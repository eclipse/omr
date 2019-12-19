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

#if !defined(OSCACHE_IMPL_HPP_INCLUDED)
#define OSCACHE_IMPL_HPP_INCLUDED

#include <cstring>

#include "OSCache.hpp"
#include "OSCacheConfigOptions.hpp"
#include "OSCacheMemoryProtector.hpp"

#include "omr.h"
#include "omrport.h"
#include "ut_omrshr.h"

typedef enum SH_CacheFileAccess {
	OMRSH_CACHE_FILE_ACCESS_ALLOWED 				= 0,
	OMRSH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED,
	OMRSH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED,
	OMRSH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED,
} SH_CacheFileAccess;

#define OMRSH_OSCACHE_CREATE 			0x1
#define OMRSH_OSCACHE_OPEXIST_DESTROY	0x2
#define OMRSH_OSCACHE_OPEXIST_STATS		0x4
#define OMRSH_OSCACHE_OPEXIST_DO_NOT_CREATE	0x8
#define OMRSH_OSCACHE_UNKNOWN -1

/*
 * The different results from attempting to open/create a cache are
 * defined below. Failure cases MUST all be less than zero.
 */
#define OMRSH_OSCACHE_OPENED 2
#define OMRSH_OSCACHE_CREATED 1
#define OMRSH_OSCACHE_FAILURE -1
#define OMRSH_OSCACHE_CORRUPT -2
#define OMRSH_OSCACHE_DIFF_BUILDID -3
#define OMRSH_OSCACHE_OUTOFMEMORY -4
#define OMRSH_OSCACHE_INUSE -5
#define OMRSH_OSCACHE_NO_CACHE -6

#define OMRSH_OSCACHE_HEADER_OK 0
#define OMRSH_OSCACHE_HEADER_WRONG_VERSION -1
#define OMRSH_OSCACHE_HEADER_CORRUPT -2
#define OMRSH_OSCACHE_HEADER_MISSING -3
#define OMRSH_OSCACHE_HEADER_DIFF_BUILDID -4
#define OMRSH_OSCACHE_SEMAPHORE_MISMATCH -5

#define OMRSH_OSCACHE_READONLY_RETRY_COUNT 10
#define OMRSH_OSCACHE_READONLY_RETRY_SLEEP_MILLIS 10

#define OSC_TRACE(configOptions, var) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_INFO, var)
#define OSC_TRACE1(configOptions, var, p1) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_INFO, var, p1)
#define OSC_TRACE2(configOptions, var, p1, p2) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_INFO, var, p1, p2)
#define OSC_ERR_TRACE(configOptions, var) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_ERROR, var)
#define OSC_ERR_TRACE1(configOptions, var, p1) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_ERROR, var, p1)
#define OSC_ERR_TRACE2(configOptions, var, p1, p2) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_ERROR, var, p1, p2)
#define OSC_ERR_TRACE4(configOptions, var, p1, p2, p3, p4) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_ERROR, var, p1, p2, p3, p4)
#define OSC_WARNING_TRACE(configOptions, var) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_WARNING, var)
#define OSC_WARNING_TRACE1(configOptions, var, p1) if (configOptions->verboseEnabled()) omrnls_printf(J9NLS_WARNING, var, p1)

#define OMRSH_MAXPATH EsMaxPath

/**
 * @struct SH_OSCache_Info
 * Information about a OSCache
 * If the information is not available, the value will be equals to @arg J9SH_OSCACHE_UNKNOWN
 */
typedef struct SH_OSCache_Info {
        char name[CACHE_ROOT_MAXLEN]; /** The name of the cache */
        UDATA os_shmid; /** Operating System specific shared memory id */
        UDATA os_semid; /** Operating System specific semaphore id */
        I_64 lastattach; /** time from which last attach has happened */
        I_64 lastdetach; /** time from which last detach has happened */
        I_64 createtime; /** time from which cache has been created */
        IDATA nattach; /** number of process attached to this region */
  //        J9PortShcVersion versionData; /** Cache version data */
  //        UDATA generation; /** cache generation number */
  //        UDATA isCompatible; /** Is the cache compatible with this VM */
        UDATA isCorrupt; /** Is set when the cache is found to be corrupt */
  //        UDATA isJavaCorePopulated; /** Is set when the javacoreData contains valid data */
  //        J9SharedClassJavacoreDataDescriptor javacoreData; /** If isCompatible is true, then extra information about the cache is availaible in here*/
} SH_OSCache_Info;

class OSCacheIterator;

/*
 * This class houses utility functions that depend on the state of
 * cache internals. Those that don't depend on cache internals are static
 * functions, and are stored in the OSCache*Utils namespaces.
 */
class OSCacheImpl: public OSCache
{
public:
  OSCacheImpl(OMRPortLibrary* library, OSCacheConfigOptions* configOptions, IDATA numLocks,
	      char* cacheName, char* cacheLocation);

  virtual ~OSCacheImpl() {
      omrthread_t self = (omrthread_t) NULL;
      omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);

      _portLibrary->port_shutdown_library(_portLibrary);
      omrthread_detach(self);
  }

  // old J9 cache comment:
  /**
   * Advise the OS to release resources used by a section of the shared classes cache
   */
  virtual void dontNeedMetadata(const void* startAddress, size_t length);

  virtual OSCacheIterator* constructCacheIterator(char* resultBuf) = 0;

  bool runningReadOnly() const {
      return _runningReadOnly;
  }

  virtual bool startup(const char* cacheName, const char* ctrlDirName) = 0;
  //  virtual void* attach();

  virtual bool started()  = 0;
  virtual void finalise() = 0;

  virtual const char* cacheLocation() {
      return _cacheLocation;
  }

  OMRPortLibrary* portLibrary() {
      return _portLibrary;
  }

  OSCacheConfigOptions* configOptions() {
      return _configOptions;
  }

  // constructs a memory protection visitor to specialize on both the
  // OSCacheRegion subclass and the memory semantics of the shared cache.
  virtual OSCacheMemoryProtector* constructMemoryProtector() = 0;

  // this is a template method (the design patterns kind, not the C++ kind)
  // that constructs a OSCacheMemoryProtector and passes the visited region
  // along to it.
  virtual IDATA setRegionPermissions(OSCacheRegion* region);

protected:
  virtual IDATA initCacheDirName(const char* ctrlDirName);//, UDATA cacheDirPermissions, I_32 openMode);
  virtual IDATA initCacheName(const char* cacheName) = 0;

  virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo) = 0;

  void initialize();

  void commonInit();
  void commonCleanup();

  virtual IDATA verifyCacheHeader() = 0;

  omrthread_t self;
  OMRPortLibrary* _portLibrary;
  //  I_32  _openMode; // now addressed by the OSCacheConfigOptions class.
  UDATA _runningReadOnly;
  IDATA _errorCode;

  IDATA _numLocks;
  // was:  char *_cacheDirName; // the path to the directory containing the cache file.
  char *_cacheLocation;  // the path, or a URI, or something, to the resource containing the cache.
  char *_cacheName; // the name of the cache file. Together with _cacheDir, we have the effective field,
                    // _cachePathName.
  char *_cachePathName; // the _cacheLocation + _cacheName, typically.
};

#endif
