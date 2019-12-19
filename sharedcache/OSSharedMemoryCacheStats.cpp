#include "OSCacheImpl.hpp"
#include "OSCacheUtils.hpp"
#include "OSSharedMemoryCacheUtils.hpp"
#include "OSSharedMemoryCacheStats.hpp"

#include "ut_omrshr.h"

IDATA OSSharedMemoryCacheStats::prepareCache(const char* cacheName, const char* cacheLocation)
{
  IDATA retval = 0;
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
  char cacheDirName[OMRSH_MAXPATH];

  OSCacheConfigOptions* configOptions = _cache->_configOptions;
  
  OSCacheUtils::getCacheDirName(OMRPORTLIB, cacheLocation, cacheDirName, OMRSH_MAXPATH, configOptions);//, OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT);
  if (getCacheStatsHelper(cacheDirName) == 0) {
    bool attachedMem = false;
    
    if (configOptions->statIterate() || configOptions->statList()) {      
      bool cacheStarted = _cache->started();
      
      if (!cacheStarted && !_cache->startup(cacheName, cacheLocation)) {
	 goto done;
      }
     
      /* Attach to the cache, so we can read the fields in the header */
//      if (NULL == _cache->attach()) {//omr_vmthread_getCurrent(vm), NULL)) {
//	_cache->cleanup();
//	goto done;
//      }       
    }      
  } else {
    retval = -1;
  }
  
 done:
  return retval;
}

void OSSharedMemoryCacheStats::shutdownCache()
{
    _cache->detach();
    _cache->cleanup();
}

IDATA
OSSharedMemoryCacheStats::getCacheStatsHelper(const char* cacheDirName)//, SH_OSCache_Info* cacheInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);//vm->_runtime->_portLibrary);
  OMRPortShmemStatistic statbuf;

  UDATA statrc = 0;
  OSCacheConfigOptions* configOptions = _cache->_configOptions;
    
  Trc_SHR_OSC_Sysv_getCacheStatsHelper_Entry(_cache->_cacheName);
  
#if !defined(WIN32)
  statrc = OSSharedMemoryCacheUtils::StatSysVMemoryHelper(OMRPORTLIB, cacheDirName, configOptions->groupPermissions(),
							  _cache->_cacheName, &statbuf);
#else
  statrc = omrshmem_stat(cacheDirName, configOptions->groupPermissions(), _cache->_cacheName, &statbuf);
#endif
  
  Trc_SHR_OSC_Sysv_getCacheStatsHelper_Exit();
  return statrc;
}

