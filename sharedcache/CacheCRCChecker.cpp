#include "CacheCRCChecker.hpp"

#include "omrutil.h"

U_32 CacheCRCChecker::computeRegionCRC()
{
  /*
   * 1535=1.5k - 1.  Chosen so that we aren't stepping on exact power of two boundaries through
   * the cache and yet we use a decent number of samples through the cache.
   * For a 16Meg cache this will cause us to take 10000 samples.
   * For a 100Meg cache this will cause us to take 68000 samples.
   */
  
  U_32 stepSize = 1535;
  U_32 regionSize = _crcFocus.region()->regionSize();

  if ((regionSize / stepSize) > _maxCRCSamples) {
    stepSize = regionSize / _maxCRCSamples;
  }

  U_32 seed = omrcrc32(0, NULL, 0);
  
  return _crcFocus.region()->computeCRC(seed, stepSize);  
}

void CacheCRCChecker::updateRegionCRC()
{
  U_32 value = 0;

//  leave this up to the owning cache.
  
//  if(cacheIsRunningReadOnly) {
//    return;
//  }

  value = computeRegionCRC();

  if (value) {
    *_crcFocus.focus() = value;
  }
}

// isRegionCRCValid computes a fresh CRC and checks it against the stored CRC.
// this policy might be overridden in a subclass. Or not, I dunno.
bool CacheCRCChecker::isRegionCRCValid()
{
  U_32 value = computeRegionCRC();
  return *_crcFocus.focus() == value;
}
