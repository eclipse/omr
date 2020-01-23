#include "OSMemoryMappedCache.hpp"
#include "OSMemoryMappedCacheMemoryProtector.hpp"

IDATA
OSMemoryMappedCacheMemoryProtector::setPermissions(OSCacheContiguousRegion& region)
{
    OMRPORT_ACCESS_FROM_OMRPORT(_osCache.portLibrary());
  
    return omrmmap_protect(region.regionStartAddress(),
                           region.regionSize(),
                           region.renderToMemoryProtectionFlags());
}
