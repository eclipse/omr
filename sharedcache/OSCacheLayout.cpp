#include "OSCacheLayout.hpp"

void OSCacheLayout::alignRegionsToPageBoundaries()
{
  for(int i = 0; i < _regions.size(); ++i) {
    _regions[i]->alignToPageBoundary(_osPageSize);
  }
}
