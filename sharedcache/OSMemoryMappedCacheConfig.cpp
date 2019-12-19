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

#include "omr.h"
#include "omrport.h"
#include "sharedconsts.h"
#include "ut_omrshr.h"

#include "OSMemoryMappedCacheConfig.hpp"

/*
PreinitConfig is this:

Notice, this is a J9 specific thing. It should be ignored within this
class, but addressed in J9MemoryMappedCache.

typedef struct OMRSharedCachePreinitConfig {
	UDATA sharedClassCacheSize;
	IDATA sharedClassInternTableNodeCount;
	IDATA sharedClassMinAOTSize;
	IDATA sharedClassMaxAOTSize;
	IDATA sharedClassMinJITSize;
	IDATA sharedClassMaxJITSize;
	IDATA sharedClassReadWriteBytes;
	IDATA sharedClassDebugAreaBytes;
	IDATA sharedClassSoftMaxBytes;
} OMRSharedCachePreinitConfig;
 */

OSMemoryMappedCacheConfig::OSMemoryMappedCacheConfig(UDATA numLocks)
  : _numLocks(numLocks)
  , _header(NULL)
  , _mapping(NULL)
  , _writeLockID(OMRSH_OSCACHE_MMAP_LOCKID_WRITELOCK)
  , _readWriteLockID(OMRSH_OSCACHE_MMAP_LOCKID_READWRITELOCK)
{
  _lockMutex = new omrthread_monitor_t[numLocks];

  // the work of this segment is accomplished in OSMemoryMappedCache::startup.
//  omrthread_t self;
//  omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);
//  
//  for(int i = 0; i < numLocks; ++i) {
//    omrthread_monitor_init(&_lockMutex[i], 0);
//  }
//
//  omrthread_detach(self);
}

IDATA OSMemoryMappedCacheConfig::getWriteLockID()
{
  // was: return J9SH_OSCACHE_MMAP_LOCKID_WRITELOCK;
  return _writeLockID;
}

IDATA OSMemoryMappedCacheConfig::getReadWriteLockID()
{
  // was: return J9SH_OSCACHE_MMAP_LOCKID_READWRITELOCK;
  return _readWriteLockID;
}

IDATA OSMemoryMappedCacheConfig::acquireLock(OMRPortLibrary* library, UDATA lockID, LastErrorInfo*)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  I_32 lockFlags = OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK;
  U_64 lockOffset, lockLength;
  I_32 rc = 0;
  I_64 startLoopTime = 0;
  I_64 endLoopTime = 0;
  UDATA loopCount = 0;

  Trc_SHR_OSC_Mmap_acquireWriteLock_Entry(lockID);

  // was: offsetof(OSCachemmap_header_version_current, dataLocks) + (lockID * sizeof(I_32));
  lockOffset = getLockOffset(lockID);
  // was: sizeof(((OSCachemmap_header_version_current *)NULL)->dataLocks[0]);
  lockLength = getLockSize(lockID);

  /* We enter a local mutex before acquiring the file lock. This is because file
   * locks only work between processes, whereas we need to lock between processes
   * AND THREADS. So we use a local mutex to lock between threads of the same JVM,
   * then a file lock for locking between different JVMs
   */
  Trc_SHR_OSC_Mmap_acquireWriteLock_entering_monitor(lockID);
  if (omrthread_monitor_enter(_lockMutex[lockID]) != 0) {
    Trc_SHR_OSC_Mmap_acquireWriteLock_failed_monitor_enter(lockID);
    return -1;
  }

  Trc_SHR_OSC_Mmap_acquireWriteLock_gettingLock(_fileHandle, lockFlags, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
  rc = omrfile_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#else
  rc = omrfile_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#endif

//  this is an example of how to handle deadlock, where two contending
//  VMs or threads each hold the mmap lock desired by the other.

//  while ((rc == -1) && (omrerror_last_error_number() == OMRPORT_ERROR_FILE_LOCK_EDEADLK)) {
//    if (++loopCount > 1) {
//      /* We time the loop so it doesn't loop forever. Try the lock algorithm below
//       * once before staring the timer. */
//      if (startLoopTime == 0) {
//	startLoopTime = omrtime_nano_time();
//      } else if (loopCount > 2) {
//	/* Loop at least twice */
//	endLoopTime = omrtime_nano_time();
//	if ((endLoopTime - startLoopTime) > ((I_64)RETRY_OBTAIN_WRITE_LOCK_MAX_MS * NANOSECS_PER_MILLISEC)) {
//	  break;
//	}
//      }
//      
//      omrthread_nanosleep(RETRY_OBTAIN_WRITE_LOCK_SLEEP_NS);
//    }
//    /* CMVC 153095: there are only three states our locks may be in if EDEADLK is detected.
//     * We can recover from cases 2 & 3 (see comments inline below). For case 1 our only option
//     * is to exit and let the caller retry.
//     */
//    if (lockID == _readWriteLockID && omrthread_monitor_owned_by_self(_lockMutex[_writeLockID]) == 1) {
//      /*	CMVC 153095: Case 1
//       * Current thread:
//       *	- Owns W monitor, W lock, RW monitor, and gets EDADLK on RW lock
//       *
//       * Notes:
//       *	- This means other JVMs caused EDEADLK becuase they are holding RW, and
//       *	  waiting on W in a sequence that gives fcntl the impression of deadlock
//       *	- If current thread owns the W monitor, it must also own the W lock
//       *	  if the call stack ended up here.
//       *
//       * Recovery:
//       *	- In this case we can't do anything but retry RW, because EDEADLK is caused by other JVMs.
//       *
//       */
//      Trc_SHR_OSC_Mmap_acquireWriteLockDeadlockMsg("Case 1: Current thread owns W lock & monitor, and RW monitor, but EDEADLK'd on RW lock");
//#if defined(WIN32) || defined(WIN64)
//      rc = omrfile_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
//#else
//      rc = omrfile_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
//#endif
//
//    } else if (lockID == _readWriteLockID) {
//      /*	CMVC 153095: Case 2
//       * Another thread:
//       *	- Owns the W monitor, and is waiting on (or owns) the W lock
//       *
//       * Current thread:
//       *	- Current thread owns the RW monitor, and gets EDEADLK on RW lock
//       *
//       * Note:
//       *  - Deadlock might caused by the order in which threads have taken taken locks, when compaired to another JVM.
//       *  - In the recovery code below the first release of the RW Monitor is to ensure SCStoreTransactions
//       *    can complete.
//       *
//       * Recovery
//       *	- Recover by trying to take the W monitor, then RW monitor and lock. This will
//       *	  resolve any EDEADLK caused by this JVM, because it ensures no thread in this JVM will hold
//       *	  the W lock.
//       */
//      Trc_SHR_OSC_Mmap_acquireWriteLockDeadlockMsg("Case 2: Current thread owns RW mon, but EDEADLK'd on RW lock");
//      omrthread_monitor_exit(_lockMutex[_readWriteLockID]);
//
//      if (omrthread_monitor_enter(_lockMutex[_writeLockID]) != 0) {
//	Trc_SHR_OSC_Mmap_acquireWriteLock_errorTakingWriteMonitor();
//	return -1;
//      }
//
//      if (omrthread_monitor_enter(_lockMutex[_readWriteLockID]) != 0) {
//	Trc_SHR_OSC_Mmap_acquireWriteLock_errorTakingWriteMonitor();
//	omrthread_monitor_exit(_lockMutex[getWriteLockID()]);
//	return -1;
//      }
//#if defined(WIN32) || defined(WIN64)
//      rc = omrfile_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
//#else
//      rc = omrfile_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
//#endif
//
//      omrthread_monitor_exit(_lockMutex[getWriteLockID()]);
//
//    } else if (lockID == _writeLockID) {
//      /*	CMVC 153095: Case 3
//       * Another thread:
//       *	- Owns RW monintor, and is waiting on (or owns) the RW lock.
//       *
//       * Current thread:
//       *	- Owns W monintor, and gets EDEADLK on W lock.
//       *
//       * Note:
//       *  - If the 'call stack' ends up here then it is known the current thread
//       *    does not own the ReadWrite lock. The shared classes code always
//       *    takes the W lock, then RW lock, OR just the RW lock.
//       *
//       * Recovery:
//       *	- In this case we recover by waiting on the RW monitor before taking the W lock. This will
//       *	  resolve any EDEADLK caused by this JVM, because it ensures no thread in this JVM will hold
//       *	  the RW lock.
//       */
//      Trc_SHR_OSC_Mmap_acquireWriteLockDeadlockMsg("Case 3: Current thread owns W mon, but EDEADLK'd on W lock");
//      if (omrthread_monitor_enter(_lockMutex[_readWriteLockID]) != 0) {
//	Trc_SHR_OSC_Mmap_acquireWriteLock_errorTakingReadWriteMonitor();
//	break;
//      }
//#if defined(WIN32) || defined(WIN64)
//      rc = omrfile_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
//#else
//      rc = omrfile_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
//#endif
//      omrthread_monitor_exit(_lockMutex[getReadWriteLockID()]);
//
//    } else {
//      Trc_SHR_Assert_ShouldNeverHappen();
//    }
//  }

  if (rc == -1) {
    Trc_SHR_OSC_Mmap_acquireWriteLock_badLock();
    omrthread_monitor_exit(_lockMutex[lockID]);
  } else {
    Trc_SHR_OSC_Mmap_acquireWriteLock_goodLock();
  }

  Trc_SHR_OSC_Mmap_acquireWriteLock_Exit(rc);
  return rc;
}

// was: acquireHeaderWriteLock
// see the comment above acquireWriteLock. It shouldn't depend on a lockID dependent thing.
IDATA OSMemoryMappedCacheConfig::acquireHeaderWriteLock(OMRPortLibrary* library, UDATA runningReadOnly,
							LastErrorInfo* lastErrorInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  I_32 lockFlags = OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK;
  U_64 lockOffset, lockLength;
  I_32 rc = 0;

  Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_Entry();

  /*
  if (NULL != lastErrorInfo) {
    lastErrorInfo->lastErrorCode = 0;
  }
  */

  if (runningReadOnly) {
    Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_ExitReadOnly();
    return 0;
  }

  lockOffset = getHeaderLockOffset();
  // sizeof(((OSCachemmap_header_version_current *)NULL)->headerLock);
  lockLength = getHeaderLockSize();

  Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_gettingLock(_fileHandle, lockFlags, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
  rc = omrfile_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#else
  rc = omrfile_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#endif

  if (-1 == rc) {
    if (NULL != lastErrorInfo) {
      lastErrorInfo->populate(library);
      /*
      lastErrorInfo->lastErrorCode = omrerror_last_error_number();
      lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
      */
    }
    Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_badLock();
  } else {
    Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_goodLock();
  }

  Trc_SHR_OSC_Mmap_acquireHeaderWriteLock_Exit(rc);
  return (IDATA)rc;
}



/**
 * Method to release the write lock on the cache header region
 *
 * Needs to be able to work with all generations
 *
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 *
 * @return 0 on success, -1 on failure
 */
IDATA
OSMemoryMappedCacheConfig::releaseHeaderWriteLock(OMRPortLibrary* library, UDATA runningReadOnly,
						  LastErrorInfo *lastErrorInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  U_64 lockOffset, lockLength;
  I_32 rc = 0;

  Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_Entry();

  //if (NULL != lastErrorInfo) {
  //  lastErrorInfo->lastErrorCode = 0;
  //}

  if (runningReadOnly) {
    Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_ExitReadOnly();
    return 0;
  }

  // (U_64)getMmapHeaderFieldOffsetForGen(generation, OSCACHEMMAP_HEADER_FIELD_HEADER_LOCK);
  lockOffset = getHeaderLockOffset();
  // sizeof(((OSCachemmap_header_version_current *)NULL)->headerLock);
  lockLength = getHeaderLockSize();

  Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_gettingLock(_fileHandle, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
  rc = omrfile_blockingasync_unlock_bytes(_fileHandle, lockOffset, lockLength);
#else
  rc = omrfile_unlock_bytes(_fileHandle, lockOffset, lockLength);
#endif
  if (-1 == rc) {
    if (NULL != lastErrorInfo) {
      lastErrorInfo->populate(library);
    //  lastErrorInfo->lastErrorCode = omrerror_last_error_number();
    //  lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
    }
    Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_badLock();
  } else {
    Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_goodLock();
  }

  Trc_SHR_OSC_Mmap_releaseHeaderWriteLock_Exit(rc);
  return (IDATA)rc;
}

IDATA OSMemoryMappedCacheConfig::releaseLock(OMRPortLibrary* library, UDATA lockID)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  U_64 lockOffset, lockLength;
  I_32 rc = 0;

  Trc_SHR_OSC_Mmap_releaseWriteLock_Entry(lockID);

  if (lockID >= _numLocks) { //J9SH_OSCACHE_MMAP_LOCK_COUNT) {
    Trc_SHR_OSC_Mmap_releaseWriteLock_BadLockID(lockID);
    return -1;
  }

  lockOffset = getLockOffset(lockID);
  lockLength = getLockSize(lockID);

  Trc_SHR_OSC_Mmap_releaseWriteLock_gettingLock(_fileHandle, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
  rc = omrfile_blockingasync_unlock_bytes(_fileHandle, lockOffset, lockLength);
#else
  rc = omrfile_unlock_bytes(_fileHandle, lockOffset, lockLength);
#endif

  if (-1 == rc) {
    Trc_SHR_OSC_Mmap_releaseWriteLock_badLock();
  } else {
    Trc_SHR_OSC_Mmap_releaseWriteLock_goodLock();
  }

  Trc_SHR_OSC_Mmap_releaseWriteLock_exiting_monitor(lockID);
  if (omrthread_monitor_exit(_lockMutex[lockID]) != 0) {
    Trc_SHR_OSC_Mmap_releaseWriteLock_bad_monitor_exit(lockID);
    rc = -1;
  }

  Trc_SHR_OSC_Mmap_releaseWriteLock_Exit(rc);
  return rc;
}

/**
 * Method to acquire the read lock on the cache attach region
 *
 * Needs to be able to work with all generations
 *
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 *
 * @return 0 on success, -1 on failure
 */
IDATA OSMemoryMappedCacheConfig::acquireAttachReadLock(OMRPortLibrary* library, LastErrorInfo* lastErrorInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  I_32 lockFlags = OMRPORT_FILE_READ_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK;
  U_64 lockOffset, lockLength;
  I_32 rc = 0;

  Trc_SHR_OSC_Mmap_acquireAttachReadLock_Entry();

  /*
  if (NULL != lastErrorInfo) {
    lastErrorInfo->lastErrorCode = 0;
  }
  */

  lockOffset = getAttachLockOffset();
  lockLength = getAttachLockSize();

  Trc_SHR_OSC_Mmap_acquireAttachReadLock_gettingLock(_fileHandle, lockFlags, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
  rc = omrfile_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#else
  rc = omrfile_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#endif

  if (-1 == rc) {
    if (NULL != lastErrorInfo) {
      lastErrorInfo->populate(library);
    }
    Trc_SHR_OSC_Mmap_acquireAttachReadLock_badLock();
  } else {
    Trc_SHR_OSC_Mmap_acquireAttachReadLock_goodLock();
  }

  Trc_SHR_OSC_Mmap_acquireAttachReadLock_Exit(rc);
  return rc;
}

/**
 * Method to try to acquire the write lock on the cache attach region
 *
 * Needs to be able to work with all generations
 *
 * This method does not wait for the lock to become available, but
 * immediately returns whether the lock has been obtained to the caller
 *
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 *
 * @return 0 on success, -1 on failure
 */
IDATA
OSMemoryMappedCacheConfig::tryAcquireAttachWriteLock(OMRPortLibrary* library)//UDATA generation)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  I_32 lockFlags = OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_NOWAIT_FOR_LOCK;
  U_64 lockOffset, lockLength;
  I_32 rc = 0;

  Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_Entry();

  lockOffset = getAttachLockOffset();
  lockLength = getAttachLockSize();

  Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_gettingLock(_fileHandle, lockFlags, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
  rc = omrfile_blockingasync_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#else
  rc = omrfile_lock_bytes(_fileHandle, lockFlags, lockOffset, lockLength);
#endif
  if (-1 == rc) {
    Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_badLock();
  } else {
    Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_goodLock();
  }

  Trc_SHR_OSC_Mmap_tryAcquireAttachWriteLock_Exit(rc);
  return rc;
}

/**
 * Method to release the write lock on the cache attach region
 *
 * Needs to be able to work with all generations
 *
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 *
 * @return 0 on success, -1 on failure
 */
IDATA
OSMemoryMappedCacheConfig::releaseAttachWriteLock(OMRPortLibrary* library) //UDATA generation)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  U_64 lockOffset, lockLength;
  I_32 rc = 0;

  Trc_SHR_OSC_Mmap_releaseAttachWriteLock_Entry();

  lockOffset = getAttachLockOffset();
  lockLength = getAttachLockSize();

  Trc_SHR_OSC_Mmap_releaseAttachWriteLock_gettingLock(_fileHandle, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
  rc = omrfile_blockingasync_unlock_bytes(_fileHandle, lockOffset, lockLength);
#else
  rc = omrfile_unlock_bytes(_fileHandle, lockOffset, lockLength);
#endif
  if (-1 == rc) {
    Trc_SHR_OSC_Mmap_releaseAttachWriteLock_badLock();
  } else {
    Trc_SHR_OSC_Mmap_releaseAttachWriteLock_goodLock();
  }

  Trc_SHR_OSC_Mmap_releaseAttachWriteLock_Exit(rc);
  return rc;
}

/**
 * Method to release the read lock on the cache attach region
 *
 * Needs to be able to work with all generations
 *
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 *
 * @return 0 on success, -1 on failure
 */
IDATA OSMemoryMappedCacheConfig::releaseAttachReadLock(OMRPortLibrary* library)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  U_64 lockOffset, lockLength;
  I_32 rc = 0;

  Trc_SHR_OSC_Mmap_releaseAttachReadLock_Entry();

  //(U_64)getMmapHeaderFieldOffsetForGen(generation, OSCACHEMMAP_HEADER_FIELD_ATTACH_LOCK);
  lockOffset = getAttachLockOffset();
  //sizeof(((OSCachemmap_header_version_current *)NULL)->attachLock);
  lockLength = getAttachLockSize();

  Trc_SHR_OSC_Mmap_releaseAttachReadLock_gettingLock(_fileHandle, lockOffset, lockLength);
#if defined(WIN32) || defined(WIN64)
  rc = omrfile_blockingasync_unlock_bytes(_fileHandle, lockOffset, lockLength);
#else
  rc = omrfile_unlock_bytes(_fileHandle, lockOffset, lockLength);
#endif

  if (-1 == rc) {
    Trc_SHR_OSC_Mmap_releaseAttachReadLock_badLock();
  } else {
    Trc_SHR_OSC_Mmap_releaseAttachReadLock_goodLock();
  }

  Trc_SHR_OSC_Mmap_releaseAttachReadLock_Exit(rc);
  return rc;
}

bool OSMemoryMappedCacheConfig::cacheFileAccessAllowed() const
{
  return _cacheFileAccess == OMRSH_CACHE_FILE_ACCESS_ALLOWED;
}

bool OSMemoryMappedCacheConfig::isCacheAccessible() const
{
  if (OMRSH_CACHE_FILE_ACCESS_ALLOWED == _cacheFileAccess) {
    return J9SH_CACHE_ACCESS_ALLOWED;
  } else if (OMRSH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED == _cacheFileAccess) {
    return J9SH_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS;
  } else {
    return J9SH_CACHE_ACCESS_NOT_ALLOWED;
  }
}

/**
 * Method to update the last attached time in a cache's header
 *
 * @param [in] headerArg  A pointer to the cache header
 *
 * @return true on success, false on failure
 * THREADING: Pre-req caller holds the cache header write lock
 */
bool
OSMemoryMappedCacheConfig::updateLastAttachedTime(OMRPortLibrary* library, UDATA runningReadOnly)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);
  Trc_SHR_OSC_Mmap_updateLastAttachedTime_Entry();

  if (runningReadOnly) {
    Trc_SHR_OSC_Mmap_updateLastAttachedTime_ReadOnly();
    return true;
  }

  I_64 newTime = omrtime_current_time_millis();
  Trc_SHR_OSC_Mmap_updateLastAttachedTime_time(newTime, _mapping->_lastAttachedTime);
  _mapping->_lastAttachedTime = newTime;

  Trc_SHR_OSC_Mmap_updateLastAttachedTime_Exit();
  return true;
}

/**
 * Method to update the last detached time in a cache's header
 *
 * @return true on success, false on failure
 * THREADING: Pre-req caller holds the cache header write lock
 */
bool
OSMemoryMappedCacheConfig::updateLastDetachedTime(OMRPortLibrary* library, UDATA runningReadOnly)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  I_64 newTime;

  Trc_SHR_OSC_Mmap_updateLastDetachedTime_Entry();

  if (runningReadOnly) {
    Trc_SHR_OSC_Mmap_updateLastDetachedTime_ReadOnly();
    return true;
  }

  newTime = omrtime_current_time_millis();
  Trc_SHR_OSC_Mmap_updateLastDetachedTime_time(newTime, _mapping->_lastDetachedTime);
  _mapping->_lastDetachedTime = newTime;

  Trc_SHR_OSC_Mmap_updateLastDetachedTime_Exit();
  return true;
}
