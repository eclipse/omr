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

#include "OSCacheConfigOptions.hpp"
#include "OSMemoryMappedCache.hpp"
#include "OSMemoryMappedCacheUtils.hpp"
#include "OSMemoryMappedCacheConfig.hpp"

/**
 * This method checks whether the group access of the shared cache file is successfully set when a new cache/snapshot is created with "groupAccess" suboption
 *
 * @param [in] portLibrary The port library
 * @param[in] fileHandle The handle of the shared cache file
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message
 * 
 * @return -1 Failed to get the stats of the file.
 * 			0 Group access is not set.
 * 			1 Group access is set.
 */
I_32
OSMemoryMappedCacheUtils::verifyCacheFileGroupAccess(OMRPortLibrary *portLibrary, IDATA fileHandle, LastErrorInfo *lastErrorInfo)
{
  I_32 rc = 1;
#if !defined(WIN32)
  struct J9FileStat statBuf;
  OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	
  memset(&statBuf, 0, sizeof(statBuf));
  if (0 == omrfile_fstat(fileHandle, &statBuf)) {
    if ((1 != statBuf.perm.isGroupWriteable)
	|| (1 != statBuf.perm.isGroupReadable)
	) {
      rc = 0;
    }
  } else {
    if (NULL != lastErrorInfo) {
      /*
      lastErrorInfo->lastErrorCode = omrerror_last_error_number();
      lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
      */
      lastErrorInfo->populate(OMRPORTLIB);
    }
    
    rc = -1;
  }

#endif /* !defined(WIN32) */
  return rc;
}

  // TODO: make this work with subclasses of OSMemoryMappedCache! The problem is, it can't construct them
  // straightaway.
// IDATA
// getCacheStats(OMRPortLibrary* library, const char* cacheDirName, const char* cacheName, SH_OSCache_Info *cacheInfo,
// 	      OSCacheConfigOptions configOptions)
// {
//   OMRPORT_ACCESS_FROM_OMRPORT(library);
//   OSMemoryMappedCache* cache = NULL;
//   
//   I_64 *timeValue;
//   I_32 inUse;
//   IDATA lockRc;
// 
//   cache = new OSMemoryMappedCache(OMRPORTLIB, cacheName, cacheDirName, OMRSH_OSCACHE_MMAP_LOCK_COUNT, configOptions);
// 
//   if (!cache->startup(cacheName, cacheDirName)) {
//     /* If that fails - try to open the cache read-only */
//     configOptions.setReadOnlyOpenMode();
// 
//     if (!cache->startup(cacheName, cacheDirName)) {
//       cache->cleanup();
//       return -1;
//     }
// 
//     inUse = OMRSH_OSCACHE_UNKNOWN;
//   } else {
//     /* Try to acquire the attach write lock. This will only succeed if noone else
//      * has the attach read lock i.e. there is noone connected to the cache */
//     lockRc = cache->tryAcquireAttachWriteLock(); // cacheInfo->generation);
//     if (0 == lockRc) {
//       Trc_SHR_OSC_Mmap_getCacheStats_cacheNotInUse();
//       inUse = 0;
//       cache->releaseAttachWriteLock(cacheInfo->generation);
//     } else {
//       Trc_SHR_OSC_Mmap_getCacheStats_cacheInUse();
//       inUse = 1;
//     }
//   }
// 
//   cacheInfo->lastattach = OMRSH_OSCACHE_UNKNOWN;
//   cacheInfo->lastdetach = OMRSH_OSCACHE_UNKNOWN;
//   cacheInfo->createtime = OMRSH_OSCACHE_UNKNOWN;
//   cacheInfo->nattach = inUse;
// 
//   /* this next big commented-outpart deals with reading in the values
//    * of the header, based on the mode the cache is in (32-bit or
//    * 64-bit addressing mode). Here we get around the same problem by
//    * calling the header's virtual functions.
//    */
// 
//   // TODO: make getCacheStats a friend function of OSMemoryMappedCache
//   // so it can call these protected functions.
//   IDATA rc = cache->internalAttach();
//   
//   if (0 != rc) {
//     cache->setError(rc);
//     cache->cleanup();
//     return -1;
//   }
//   
//   if ((timeValue = (I_64*) cache->_config->getLastAttachTimeLocation())) {
//     cacheInfo->lastattach = *timeValue;
//   }
//   if ((timeValue = (I_64*) cache->_config->getLastDetachTimeLocation())) {
//     cacheInfo->lastdetach = *timeValue;
//   }
//   if ((timeValue = (I_64*) cache->_config->getLastCreateTimeLocation())) {
//     cacheInfo->createtime = *timeValue;
//   }
// 
//   cache->internalDetach();
// 
// // TODO: I've put this off until CacheMap is ported.
//   
// //  if (SHR_STATS_REASON_ITERATE == reason) {
// //    getCacheStatsCommon(vm, cache, cacheInfo);
// //  }
// 
// //	Trc_SHR_OSC_Mmap_getCacheStats_Exit(cacheInfo->os_shmid,
// //					    cacheInfo->os_semid,
// //					    cacheInfo->lastattach,
// //					    cacheInfo->lastdetach,
// //					    cacheInfo->createtime,
// //					    cacheInfo->nattach,
// //					    cacheInfo->versionData.cacheType);
// 	/* Note that generation is not set here. This could be determined by parsing the filename,
// 	 * but is currently set by the caller */
// 	cache->cleanup();
// 	return 0;
//   
//   /* CMVC 177634: Skip calling internalAttach() when destroying the cache */
//   //  if (!configOption.openToDestroyExistingCache()) {
//     /* The offset of fields createTime, lastAttachedTime, lastDetachedTime in struct OSCache_mmap_header2 are different on 32-bit and 64-bit caches. 
//      * This depends on OSCachemmap_header_version_current and OSCache_header_version_current not changing in an incompatible way.
//      */
// 
// //    		if (OMRSH_ADDRMODE == cacheInfo->versionData.addrmode) {
// //			IDATA rc;
// //			/* Attach to the cache, so we can read the fields in the header */
// //			rc = cache->internalAttach(false, cacheInfo->generation);
// //			if (0 != rc) {
// //				cache->setError(rc);
// //				cache->cleanup();
// //				return -1;
// //			}
// //			cacheHeader = cache->_headerStart;
// //
// //			/* Read the fields from the header and populate cacheInfo */
// //			if ((timeValue = (I_64*)getMmapHeaderFieldAddressForGen(cacheHeader, cacheInfo->generation, OSCACHEMMAP_HEADER_FIELD_LAST_ATTACHED_TIME))) {
// //				cacheInfo->lastattach = *timeValue;
// //			}
// //			if ((timeValue = (I_64*)getMmapHeaderFieldAddressForGen(cacheHeader, cacheInfo->generation, OSCACHEMMAP_HEADER_FIELD_LAST_DETACHED_TIME))) {
// //				cacheInfo->lastdetach = *timeValue;
// //			}
// //			if ((timeValue = (I_64*)getMmapHeaderFieldAddressForGen(cacheHeader, cacheInfo->generation, OSCACHEMMAP_HEADER_FIELD_CREATE_TIME))) {
// //				cacheInfo->createtime = *timeValue;
// //			}
// //			if (SHR_STATS_REASON_ITERATE == reason) {
// //				getCacheStatsCommon(vm, cache, cacheInfo);
// //			}
// //			cache->internalDetach(cacheInfo->generation);
// //		}
// //  */
// //  }
// }
