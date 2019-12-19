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

#if !defined(OSCACHE_HPP_INCLUDED)
#define OSCACHE_HPP_INCLUDED

#include "OSCacheConfigOptions.hpp"
#include "OSCacheRegion.hpp"
#include "OSCacheRegionInitializer.hpp"
#include "OSCacheRegionSerializer.hpp"

#include "omr.h"
#include "sharedconsts.h"

// this was formerly a struct with just two fields, but I can see no
// reason it should not be a class -- it's not ever used in a C
// exclusive context. Also, the block of code that populates its
// fields is repeated verbatim throughout the SCC, so why shouldn't it
// be a member function?
struct LastErrorInfo
{
  I_32 _lastErrorCode;
  const char* _lastErrorMsg;

  LastErrorInfo()
    : _lastErrorCode(0)
    , _lastErrorMsg(NULL)
  {}

  void populate(OMRPortLibrary* library) {
    OMRPORT_ACCESS_FROM_OMRPORT(library);
    
    _lastErrorCode = omrerror_last_error_number();
    _lastErrorMsg = omrerror_last_error_message();
  }
};

class OSCache {
public:
  OSCache(OSCacheConfigOptions* configOptions)
    : _configOptions(configOptions)
  {}

  virtual ~OSCache() {}
  
  // get the data and total sizes of the cache.
  virtual U_32 getDataSize() = 0;
  virtual U_32 getTotalSize() = 0;

  // moduleName, whaaa?? I'm not sure.
  virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo) = 0;

  // this as well.
  virtual SH_CacheAccess isCacheAccessible(void) const { return J9SH_CACHE_ACCESS_ALLOWED; }

  // this belongs to the OSCache hierarchy at the highest level.
  virtual void getCorruptionContext(IDATA *corruptionCode, UDATA *corruptValue) {
	if (NULL != corruptionCode) {
		*corruptionCode = _corruptionCode;
	}
	
	if (NULL != corruptValue) {
		*corruptValue = _corruptValue;
	}    
  }
  
  virtual void setCorruptionContext(IDATA corruptionCode, UDATA corruptValue) {
    _corruptionCode = corruptionCode;
    _corruptValue = corruptValue;
  }

  // this is a factory method that produces visitor objects.
  // these visitors codify how to serialize (resp. initialize) a region.

  virtual OSCacheRegionSerializer* constructSerializer() = 0;
  virtual OSCacheRegionInitializer* constructInitializer() = 0;

  // startup routines the Composite Cache expects.
  virtual void* attach() = 0;
  virtual bool startup(const char* cacheName, const char* ctrlDirName) = 0;
  
  // cleanup resources used by the cache. I'm half-convinced this should occupy a
  // Destruction methods.
  virtual void cleanup() = 0;
  // deletes the shared class. contains an option to suppress verbose trace messages.
  virtual IDATA destroy(bool suppressVerbose, bool isReset = false) = 0;
  // reports on the contents of error codes and the corruption context.
  virtual IDATA getError() = 0;
  // calls routines to finalize/shutdown the cache when OMR is exiting.
  virtual void runExitProcedure() = 0;

  // get the lock capabilities of the cache.
  virtual IDATA getLockCapabilities() = 0;
  // the region object has an internal value, "flags", that specify
  // permissions settings, and a few other things.
  virtual IDATA setRegionPermissions(OSCacheRegion* region) = 0;
  // returns the size of the smallest region on which permissions can be set. A return value of 0
  // means the setting of permissions is unsupported.
  virtual UDATA getPermissionsRegionGranularity() = 0;

protected:
  IDATA _corruptionCode;
  UDATA _corruptValue;

  OSCacheConfigOptions* _configOptions;
};

#endif
