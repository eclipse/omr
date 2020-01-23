#include "OSCacheContiguousRegion.hpp"
#include "OSCacheMemoryProtector.hpp"

#include "omrutil.h"

OSCacheContiguousRegion::OSCacheContiguousRegion(OSCacheLayout* layout,
                                                 int regionID,
                                                 bool pageBoundaryAligned)
    : OSCacheRegion(layout, regionID)
    , _regionStart(NULL)
    , _regionSize(0)
    , _pageBoundaryAligned(pageBoundaryAligned)
{}

bool
OSCacheContiguousRegion::adjustRegionStartAndSize(void* blockAddress, uintptr_t size)
{
    _regionStart = blockAddress;
    _regionSize = size;
    return true;
}

bool
OSCacheContiguousRegion::alignToPageBoundary(UDATA osPageSize)
{
    if(_pageBoundaryAligned) {
        if(osPageSize > 0) {
            _regionStart = (void*) ROUND_UP_TO(osPageSize, (UDATA) _regionStart);

            UDATA naiveEnd   = (UDATA) _regionStart + _regionSize;
            UDATA roundedEnd = ROUND_DOWN_TO(osPageSize, naiveEnd);

            _regionSize = roundedEnd - (UDATA) _regionStart;
            _layout->notifyRegionSizeAdjustment(*this);
        }

        return true;
    } else {
        return false;
    }
}

bool
OSCacheContiguousRegion::isAddressInRegion(void* itemAddress)
{
    return isBlockInRegion(itemAddress, 0);
}

bool
OSCacheContiguousRegion::isBlockInRegion(void* itemAddress, UDATA itemSize)
{
    return (void*) ((UDATA) regionStartAddress() + regionSize()) >= (void*) ((UDATA) itemAddress + itemSize)
        && regionStartAddress() <= itemAddress;
}

void*
OSCacheContiguousRegion::regionEnd()
{
    return (void*) ((UDATA)_regionStart + _regionSize);
}

void*
OSCacheContiguousRegion::regionStartAddress() const
{
    return _regionStart;
}

UDATA
OSCacheContiguousRegion::regionSize() const
{
    return _regionSize;
}

UDATA
OSCacheContiguousRegion::renderToMemoryProtectionFlags()
{
    return _protectionOptions->renderToFlags();
}

IDATA
OSCacheContiguousRegion::setPermissions(OSCacheMemoryProtector* protector)
{
    return protector->setPermissions(*this);
}

U_32
OSCacheContiguousRegion::computeCRC(U_32 seed, U_32 stepSize)
{
    return omrcrcSparse32(seed, (U_8*) _regionStart, (U_32) _regionSize, stepSize);
}
