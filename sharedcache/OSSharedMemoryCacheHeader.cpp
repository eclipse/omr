#include "OSCacheConfig.hpp"
#include "OSSharedMemoryCacheConfig.hpp"

void
OSSharedMemoryCacheHeader::create(OMRPortLibrary* library, bool inDefaultControlDir)
{
  *_mapping = regionStartAddress();
  
  OSSharedMemoryCacheHeaderMapping* mapping = _mapping->baseMapping();
  memset(mapping, 0, mapping->size());

  strncpy(mapping->_eyecatcher, OMRSH_OSCACHE_SYSV_EYECATCHER, OMRSH_OSCACHE_SYSV_EYECATCHER_LENGTH);
  refresh(library, mapping, inDefaultControlDir);
}

void OSSharedMemoryCacheHeader::refresh(OMRPortLibrary* library, OSSharedMemoryCacheHeaderMapping* mapping,
					bool inDefaultControlDir)
{
  OMRPORT_ACCESS_FROM_OMRPORT(library);

  mapping->_createTime = omrtime_current_time_millis();
  mapping->_inDefaultControlDir = inDefaultControlDir;
}

void OSSharedMemoryCacheHeader::refresh(OMRPortLibrary* library, bool inDefaultControlDir)
{
  *_mapping = regionStartAddress();  
  refresh(library, _mapping->baseMapping(), inDefaultControlDir);
}

void OSSharedMemoryCacheHeader::serialize(OSCacheRegionSerializer* serializer)
{
  return serializer->serialize(this);
}

void OSSharedMemoryCacheHeader::initialize(OSCacheRegionInitializer* initializer)
{
  return initializer->initialize(this);
}
