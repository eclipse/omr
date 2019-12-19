#include "OSSharedMemoryCache.hpp"
#include "OSSharedMemoryCacheMemoryProtector.hpp"

IDATA OSSharedMemoryCacheMemoryProtector::setPermissions(OSCacheContiguousRegion& region)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_osCache.portLibrary());
  
  return omrshmem_protect(_osCache.cacheLocation(), _osCache.configOptions()->groupPermissions(),
			  region.regionStartAddress(), region.regionSize(),
			  region.renderToMemoryProtectionFlags());
}
